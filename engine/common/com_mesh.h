#ifdef __cplusplus
extern "C" {
#endif

#include "hash.h"
#include "shader.h"

#ifdef SKELETALMODELS
#include <stdlib.h>
#endif

#ifdef HALFLIFEMODELS
#include "model_hl.h"
#endif

struct galiasinfo_s; //per-surface info.

#ifdef NONSKELETALMODELS
//a single pose within an animation (note: always refered to via a framegroup, even if there's only one frame in that group).
typedef struct
{
	vecV_t *ofsverts;
#ifndef SERVERONLY
	vec3_t *ofsnormals;
	vec3_t *ofstvector;
	vec3_t *ofssvector;

	vboarray_t vboverts;
	vboarray_t vbonormals;
	vboarray_t vbosvector;
	vboarray_t vbotvector;
#endif

	vec3_t		scale;
	vec3_t		scale_origin;
} galiaspose_t;
#endif

typedef struct galiasevent_s
{
	struct galiasevent_s *next;
	float timestamp;
	int code;
	char *data;
} galiasevent_t;

//a frame group (aka: animation)
typedef struct galiasanimation_s
{
#ifdef SKELETALMODELS
	skeltype_t skeltype;	//for models with transforms, states that bones need to be transformed from their parent.
							//this is actually bad, and can result in bones shortening as they interpolate.
	float *(QDECL *GetRawBones)(const struct galiasinfo_s *mesh, const struct galiasanimation_s *a, float time, float *bonematrixstorage, int numbones);
	void *boneofs;	//numposes*12*numbones
#endif
	qboolean loop;
	int numposes;
	//float *poseendtime;	//first starts at 0, anim duration is poseendtime[numposes-1]
	float rate;				//average framerate of animation.
#ifdef NONSKELETALMODELS
	galiaspose_t *poseofs;
#endif
	galiasevent_t *events;
	char name[64];
} galiasanimation_t;

typedef struct galiasbone_s galiasbone_t;
#ifdef SKELETALMODELS
struct galiasbone_s
{
	char name[32];
	int parent;
//	float radius;
	float inverse[12];
};

typedef struct
{
	//should be load-time only
	//use of this prevents the use of glsl acceleration. the framerate loss is of the order of 90%
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

typedef struct
{
	shader_t *shader;
	qbyte *texels;	//this is 8bit for frame 0 only. only valid in q1 models without replacement textures, used for colourising player skins.
	const char *defaultshader;
	char shadername[MAX_QPATH];
	texnums_t texnums;
} skinframe_t;
typedef struct
{
	int skinwidth;
	int skinheight;
	float skinspeed;
	int numframes;
	skinframe_t *frame;
	char name[MAX_QPATH];
} galiasskin_t;

typedef struct
{
	char name[MAX_QPATH];
	texnums_t texnum;
	unsigned int tcolour;
	unsigned int bcolour;
	unsigned int pclass;
	int skinnum;
	unsigned int subframe;
	bucket_t bucket;
} galiascolourmapped_t;
#else
typedef void galiasskin_t;
#endif

typedef struct
{
	char name[64];
	vec3_t org;
	float ang[3][3];
} md3tag_t;

typedef struct galiasinfo_s
{
	char surfacename[MAX_QPATH];
	unsigned short geomset;
	unsigned short geomid;
	index_t *ofs_indexes;
	int numindexes;

	//for hitmodel
	unsigned int contents;		//default CONTENTS_BODY
	q2csurface_t csurface;		//flags, and also collision name, if useful...
	unsigned int surfaceid;		//the body reported to qc via trace_surface

	float	mindist;
	float	maxdist;

	int *ofs_trineighbours;
	float lerpcutoff;	//hack. should probably be part of the entity structure, but I really don't want new models (and thus code) to have access to this ugly inefficient hack. make your models properly in the first place.

	int numskins;
//#ifndef SERVERONLY
	galiasskin_t *ofsskins;
//#endif

	int shares_verts;	//used with models with two shaders using the same vertex. set to the surface number to inherit from (or itself).
	int shares_bones;	//use last mesh's bones. set to the surface number to inherit from (or itself).

	int numverts;

//#ifndef SERVERONLY
	vec2_t *ofs_st_array;
	vec2_t *ofs_lmst_array;
	vec4_t *ofs_rgbaf;
	byte_vec4_t *ofs_rgbaub;
//#endif

	int numanimations;
	galiasanimation_t *ofsanimations;

	struct galiasinfo_s *nextsurf;

#ifdef SKELETALMODELS
	boneidx_t *bonemap;		//filled in automatically if our mesh has more gpu bones than we can support
	unsigned int mappedbones;

	float *baseframeofs;	/*non-heirachical*/
	int numbones;
	galiasbone_t *ofsbones;

	vecV_t *ofs_skel_xyz;
	vec3_t *ofs_skel_norm;
	vec3_t *ofs_skel_svect;
	vec3_t *ofs_skel_tvect;
	bone_vec4_t *ofs_skel_idx;
	vec4_t *ofs_skel_weight;

	vboarray_t vbo_skel_verts;
	vboarray_t vbo_skel_normals;
	vboarray_t vbo_skel_svector;
	vboarray_t vbo_skel_tvector;
	vboarray_t vbo_skel_bonenum;
	vboarray_t vbo_skel_bweight;
#endif
	vboarray_t vboindicies;
	vboarray_t vbotexcoords;
	vboarray_t vborgba;	//yeah, just you try reading THAT as an actual word.
	void *vbomem;
	void *ebomem;

//these exist only in the root mesh.
#ifdef MD3MODELS
	int numtagframes;
	int numtags;
	md3tag_t *ofstags;
#else
	FTE_DEPRECATED int numtagframes;
	FTE_DEPRECATED int numtags;
	FTE_DEPRECATED md3tag_t *ofstags;
#endif

	void *ctx;				//loader-specific stuff. must be ZG_Malloced if it lasts beyond the loader.
	unsigned int warned;	//passed around at load time, so we don't spam warnings
} galiasinfo_t;

struct terrainfuncs_s;
typedef struct modplugfuncs_s
{
	int version;

	//format handling
	int (QDECL *RegisterModelFormatText)(const char *formatname, char *magictext, qboolean (QDECL *load) (struct model_s *mod, void *buffer, size_t fsize));
	int (QDECL *RegisterModelFormatMagic)(const char *formatname, unsigned int magic, qboolean (QDECL *load) (struct model_s *mod, void *buffer, size_t fsize));
	void (QDECL *UnRegisterModelFormat)(int idx);
	void (QDECL *UnRegisterAllModelFormats)(void);

	//util
	void *(QDECL *ZG_Malloc)(zonegroup_t *ctx, size_t size);	//ctx=&mod->memgroup and the data will be freed when the model is freed.
	void (QDECL *StripExtension) (const char *in, char *out, int outlen);

	//matrix maths
	void (QDECL *ConcatTransforms) (const float in1[3][4], const float in2[3][4], float out[3][4]);
	void (QDECL *M3x4_Invert) (const float *in1, float *out);
	void (QDECL *VectorAngles)(const float *forward, const float *up, float *result, qboolean meshpitch);
	void (QDECL *AngleVectors)(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
	void (QDECL *GenMatrixPosQuat4Scale)(const vec3_t pos, const vec4_t quat, const vec3_t scale, float result[12]);

	//bone stuff
	void (QDECL *ForceConvertBoneData)(skeltype_t sourcetype, const float *sourcedata, size_t bonecount, galiasbone_t *bones, skeltype_t desttype, float *destbuffer, size_t destbonecount);

	//texturing
	image_t *(QDECL *GetTexture)(const char *identifier, const char *subpath, unsigned int flags, void *fallbackdata, void *fallbackpalette, int fallbackwidth, int fallbackheight, uploadfmt_t fallbackfmt);
	void (QDECL *AccumulateTextureVectors)(vecV_t *const vc, vec2_t *const tc, vec3_t *nv, vec3_t *sv, vec3_t *tv, const index_t *idx, int numidx, qboolean calcnorms);
	void (QDECL *NormaliseTextureVectors)(vec3_t *n, vec3_t *s, vec3_t *t, int v, qboolean calcnorms);

	model_t *(QDECL *GetModel)(const char *identifier, enum mlverbosity_e verbosity);
#define plugmodfuncs_name "Models"
} plugmodfuncs_t;
#define MODPLUGFUNCS_VERSION 1

#ifdef SKELETALMODELS
void Alias_TransformVerticies(float *bonepose, galisskeletaltransforms_t *weights, int numweights, vecV_t *xyzout, vec3_t *normout);
void QDECL Alias_ForceConvertBoneData(skeltype_t sourcetype, const float *sourcedata, size_t bonecount, galiasbone_t *bones, skeltype_t desttype, float *destbuffer, size_t destbonecount);
#endif
qboolean Alias_GAliasBuildMesh(mesh_t *mesh, vbo_t **vbop, galiasinfo_t *inf, int surfnum, entity_t *e, qboolean allowskel);
void Mod_DestroyMesh(galiasinfo_t *galias);
void Alias_FlushCache(void);
void Alias_Shutdown(void);
void Alias_Register(void);
shader_t *Mod_ShaderForSkin(model_t *model, int surfaceidx, int num);
const char *Mod_SkinNameForNum(model_t *model, int surfaceidx, int num);
const char *Mod_SurfaceNameForNum(model_t *model, int num);
const char *Mod_FrameNameForNum(model_t *model, int surfaceidx, int num);
const char *Mod_SkinNameForNum(model_t *model, int surfaceidx, int num);
qboolean Mod_FrameInfoForNum(model_t *model, int surfaceidx, int num, char **name, int *numframes, float *duration, qboolean *loop);

void Mod_DoCRC(model_t *mod, char *buffer, int buffersize);

void QDECL Mod_AccumulateTextureVectors(vecV_t *const vc, vec2_t *const tc, vec3_t *nv, vec3_t *sv, vec3_t *tv, const index_t *idx, int numidx, qboolean calcnorms);
void Mod_AccumulateMeshTextureVectors(mesh_t *mesh);
void QDECL Mod_NormaliseTextureVectors(vec3_t *n, vec3_t *s, vec3_t *t, int v, qboolean calcnorms);
void R_Generate_Mesh_ST_Vectors(mesh_t *mesh);

#ifdef __cplusplus
};
#endif