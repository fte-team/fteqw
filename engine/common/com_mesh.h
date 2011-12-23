
#include "hash.h"
#include "shader.h"

#if defined(ZYMOTICMODELS) || defined(MD5MODELS)
#define SKELETALMODELS
#include <stdlib.h>
#endif

int HLMod_BoneForName(model_t *mod, char *name);
int HLMod_FrameForName(model_t *mod, char *name);

typedef struct {
	int ofs_indexes;
	int numindexes;

	int ofs_trineighbours;

	int numskins;
#ifndef SERVERONLY
	int ofsskins;
#endif

	int shares_verts;	//used with models with two shaders using the same vertex. set to the surface number to inherit from (or itself).
	int shares_bones;	//use last mesh's bones. set to the surface number to inherit from (or itself).

	int numverts;

#ifndef SERVERONLY
	int ofs_st_array;
#endif

	int groups;
	int groupofs;
	int baseframeofs;	/*non-heirachical*/

	int nextsurf;

#ifdef SKELETALMODELS
	int numbones;
	int ofsbones;
	int numswtransforms;
	int ofsswtransforms;

	int ofs_skel_xyz;
	int ofs_skel_norm;
	int ofs_skel_svect;
	int ofs_skel_tvect;
	int ofs_skel_idx;
	int ofs_skel_weight;
#endif

//these exist only in the root mesh.
	int numtagframes;
	int numtags;
	int ofstags;
} galiasinfo_t;

//frame is an index into this
typedef struct {
#ifdef SKELETALMODELS
	qboolean isheirachical;	//for models with transforms, states that bones need to be transformed from their parent.
							//this is actually bad, and can result in bones shortening as they interpolate.
#endif
	qboolean loop;
	int numposes;
	float rate;
	int poseofs;
	char name[64];
} galiasgroup_t;

typedef struct {
	int ofsverts;
#ifndef SERVERONLY
	int ofsnormals;
	int ofstvector;
	int ofssvector;
#endif

	vec3_t		scale;
	vec3_t		scale_origin;
} galiaspose_t;

typedef struct galiasbone_s galiasbone_t;
#ifdef SKELETALMODELS
struct galiasbone_s {
	char name[32];
	int parent;
	float inverse[12];
};

typedef struct {
	//skeletal poses refer to this.
	int vertexindex;
	int boneindex;
	vec4_t org;
#ifndef SERVERONLY
	vec3_t normal;
#endif
} galisskeletaltransforms_t;
#endif

//we can't be bothered with animating skins.
//We'll load up to four of them but after that you're on your own
#ifndef SERVERONLY
typedef struct {
	int skinwidth;
	int skinheight;
	int ofstexels;	//this is 8bit for frame 0 only. only valid in q1 models without replacement textures, used for colourising player skins.
	float skinspeed;
	int texnums;
	int ofstexnums;
	char name [MAX_QPATH];
} galiasskin_t;

typedef struct {
	char name[MAX_QPATH];
	texnums_t texnum;
	unsigned int tcolour;
	unsigned int bcolour;
	unsigned int pclass;
	int skinnum;
	unsigned int subframe;
	bucket_t bucket;
} galiascolourmapped_t;
#endif

float *Alias_GetBonePositions(galiasinfo_t *inf, framestate_t *fstate, float *buffer, int buffersize, qboolean renderable);
#ifdef SKELETALMODELS
void Alias_TransformVerticies(float *bonepose, galisskeletaltransforms_t *weights, int numweights, vecV_t *xyzout, vec3_t *normout);
#endif
qboolean Alias_GAliasBuildMesh(mesh_t *mesh, galiasinfo_t *inf, int surfnum, entity_t *e, qboolean allowskel);
void Alias_FlushCache(void);

void Mod_DoCRC(model_t *mod, char *buffer, int buffersize);

qboolean Mod_LoadQ1Model (model_t *mod, void *buffer);
#ifdef MD2MODELS
	qboolean Mod_LoadQ2Model (model_t *mod, void *buffer);
#endif
#ifdef MD3MODELS
	qboolean Mod_LoadQ3Model(model_t *mod, void *buffer);
#endif
#ifdef ZYMOTICMODELS
	qboolean Mod_LoadZymoticModel(model_t *mod, void *buffer);
#endif
#ifdef DPMMODELS
	qboolean Mod_LoadDarkPlacesModel(model_t *mod, void *buffer);
#endif
#ifdef PSKMODELS
	qboolean Mod_LoadPSKModel(model_t *mod, void *buffer);
#endif
#ifdef INTERQUAKEMODELS
	qboolean Mod_LoadInterQuakeModel(model_t *mod, void *buffer);
#endif
#ifdef MD5MODELS
	qboolean Mod_LoadMD5MeshModel(model_t *mod, void *buffer);
	qboolean Mod_LoadCompositeAnim(model_t *mod, void *buffer);
#endif
#ifdef MAP_PROC 
	qboolean Mod_LoadMap_Proc(model_t *mode, void *buffer);
#endif

void Mod_AccumulateTextureVectors(vecV_t *vc, vec2_t *tc, vec3_t *nv, vec3_t *sv, vec3_t *tv, index_t *idx, int numidx);
void Mod_AccumulateMeshTextureVectors(mesh_t *mesh);
void Mod_NormaliseTextureVectors(vec3_t *n, vec3_t *s, vec3_t *t, int v);
