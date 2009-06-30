
#include "hash.h"
#include "shader.h"

#if defined(ZYMOTICMODELS) || defined(MD5MODELS)
#define SKELETALMODELS
#include <stdlib.h>
#endif

#define MAX_BONES 256


typedef struct {
	int ofs_indexes;
	int numindexes;

	int ofs_trineighbours;

	int numskins;
#ifndef SERVERONLY
	int ofsskins;
#endif

	qboolean sharesverts;	//used with models with two shaders using the same vertex - use last mesh's verts
	qboolean sharesbones;	//use last mesh's bones (please, never set this on the first mesh!)

	int numverts;

#ifndef SERVERONLY
	int ofs_st_array;
#endif

	int groups;
	int groupofs;

	int nextsurf;

#ifdef SKELETALMODELS
	int numbones;
	int ofsbones;
	int numtransforms;
	int ofstransforms;
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
#endif

	vec3_t		scale;
	vec3_t		scale_origin;
} galiaspose_t;

#ifdef SKELETALMODELS
typedef struct {
	char name[32];
	int parent;
} galiasbone_t;

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
	int base;
	int bump;
	int fullbright;
	int upperoverlay;
	int loweroverlay;

#ifdef Q3SHADERS
	shader_t *shader;
#endif
} galiastexnum_t;

typedef struct {
	char name[MAX_QPATH];
	galiastexnum_t texnum;
	unsigned int tcolour;
	unsigned int bcolour;
	int skinnum;
	bucket_t bucket;
} galiascolourmapped_t;
#endif

float *Alias_GetBonePositions(galiasinfo_t *inf, framestate_t *fstate, float *buffer, int buffersize);
#ifdef SKELETALMODELS
void Alias_TransformVerticies(float *bonepose, galisskeletaltransforms_t *weights, int numweights, float *xyzout, float *normals);
#endif
qboolean Alias_GAliasBuildMesh(mesh_t *mesh, galiasinfo_t *inf, 
									entity_t *e,
								  float alpha, qboolean nolightdir);

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
	qboolean Mod_LoadDarkPlacesModel(model_t *mod, void *buffer);
#endif
#ifdef MD5MODELS
	qboolean Mod_LoadMD5MeshModel(model_t *mod, void *buffer);
	qboolean Mod_LoadCompositeAnim(model_t *mod, void *buffer);
#endif
