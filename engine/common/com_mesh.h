
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


