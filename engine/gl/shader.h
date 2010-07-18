#ifndef SHADER_H
#define SHADER_H
typedef void (shader_gen_t)(char *name, shader_t*, const void *args);

#define SHADER_PASS_MAX	8
#define SHADER_MAX_TC_MODS	8
#define SHADER_DEFORM_MAX	8
#define SHADER_MAX_ANIMFRAMES	8
#define SHADER_ANIM_FRAMES_MAX 16

#define SHADER_PROGPARMS_MAX 16

typedef enum {
	SHADER_BSP,
	SHADER_BSP_VERTEX,
	SHADER_BSP_FLARE,
	SHADER_MD3,
	SHADER_2D
} shadertype_t;

typedef enum {
	SHADER_SORT_NONE,
	SHADER_SORT_PORTAL,
	SHADER_SORT_SKY,
	SHADER_SORT_OPAQUE,
	SHADER_SORT_DECAL,
	SHADER_SORT_SEETHROUGH,
	SHADER_SORT_BANNER,
	SHADER_SORT_UNDERWATER,
	SHADER_SORT_BLEND,
	SHADER_SORT_ADDITIVE,
	SHADER_SORT_NEAREST
} shadersort_t;

typedef enum {
	MF_NONE			= 1<<0,
	MF_NORMALS		= 1<<1,
	MF_TRNORMALS	= 1<<2,
	MF_COLORS		= 1<<3,
	MF_STCOORDS		= 1<<4,
	MF_LMCOORDS		= 1<<5,
	MF_NOCULL		= 1<<6,
	MF_NONBATCHED	= 1<<7
} meshfeatures_t;

//colour manipulation
typedef struct
{
    enum {
		SHADER_FUNC_SIN,
		SHADER_FUNC_TRIANGLE,
		SHADER_FUNC_SQUARE,
		SHADER_FUNC_SAWTOOTH,
		SHADER_FUNC_INVERSESAWTOOTH,
		SHADER_FUNC_NOISE,
		SHADER_FUNC_CONSTANT
	} type;				// SHADER_FUNC enum
    float			args[4];			// offset, amplitude, phase_offset, rate
} shaderfunc_t;

//tecture coordinate manipulation
typedef struct 
{
	enum {
		SHADER_TCMOD_NONE,		//bug
		SHADER_TCMOD_SCALE,		//some sorta tabled deformation
		SHADER_TCMOD_SCROLL,	//boring moving texcoords with time
		SHADER_TCMOD_STRETCH,	//constant factor
		SHADER_TCMOD_ROTATE,
		SHADER_TCMOD_MAX,
		SHADER_TCMOD_TRANSFORM,
		SHADER_TCMOD_TURB
	} type;
	float			args[6];
} tcmod_t;

//vertex positioning manipulation.
typedef struct
{
	enum {
		DEFORMV_NONE,		//bug
		DEFORMV_MOVE,
		DEFORMV_WAVE,
		DEFORMV_NORMAL,
		DEFORMV_BULGE,
		DEFORMV_AUTOSPRITE,
		DEFORMV_AUTOSPRITE2,
		DEFORMV_PROJECTION_SHADOW
	} type;
    float			args[4];
    shaderfunc_t	func;
} deformv_t;

enum
{
	/*source and dest factors match each other for easier parsing
	  but they're not meant to ever be set on the shader itself
	  NONE is also invalid, and is used to signify disabled, it should never be set on only one
	*/
	SBITS_SRCBLEND_NONE					= 0x00000000,
	SBITS_SRCBLEND_ZERO					= 0x00000001,
	SBITS_SRCBLEND_ONE					= 0x00000002,
	SBITS_SRCBLEND_DST_COLOR			= 0x00000003,
	SBITS_SRCBLEND_ONE_MINUS_DST_COLOR	= 0x00000004,
	SBITS_SRCBLEND_SRC_ALPHA			= 0x00000005,
	SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA	= 0x00000006,
	SBITS_SRCBLEND_DST_ALPHA			= 0x00000007,
	SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA	= 0x00000008,
	SBITS_SRCBLEND_SRC_COLOR_INVALID			= 0x00000009,
	SBITS_SRCBLEND_ONE_MINUS_SRC_COLOR_INVALID	= 0x0000000a,
	SBITS_SRCBLEND_ALPHA_SATURATE		= 0x0000000b,
#define SBITS_SRCBLEND_BITS				  0x0000000f

	/*must match src factors, just shifted 4*/
	SBITS_DSTBLEND_NONE					= 0x00000000,
	SBITS_DSTBLEND_ZERO					= 0x00000010,
	SBITS_DSTBLEND_ONE					= 0x00000020,
	SBITS_DSTBLEND_DST_COLOR_INVALID			= 0x00000030,
	SBITS_DSTBLEND_ONE_MINUS_DST_COLOR_INVALID	= 0x00000040,
	SBITS_DSTBLEND_SRC_ALPHA			= 0x00000050,
	SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA	= 0x00000060,
	SBITS_DSTBLEND_DST_ALPHA			= 0x00000070,
	SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA	= 0x00000080,
	SBITS_DSTBLEND_SRC_COLOR			= 0x00000090,
	SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR	= 0x000000a0,
	SBITS_DSTBLEND_ALPHA_SATURATE_INVALID		= 0x000000b0,
#define SBITS_DSTBLEND_BITS				  0x000000f0

#define SBITS_BLEND_BITS				(SBITS_SRCBLEND_BITS|SBITS_DSTBLEND_BITS)

	SBITS_ATEST_NONE					= 0x00000000,
	SBITS_ATEST_GT0						= 0x00000100,
	SBITS_ATEST_LT128					= 0x00000200,
	SBITS_ATEST_GE128					= 0x00000300,
#define SBITS_ATEST_BITS				  0x00000f00

	SBITS_MISC_DEPTHWRITE				= 0x00001000,
	SBITS_MISC_NODEPTHTEST				= 0x00002000,

	SBITS_MISC_DEPTHEQUALONLY			= 0x00004000,
	SBITS_MISC_DEPTHCLOSERONLY			= 0x00008000,

//	SBITS_MISC_POLYFILL_LINES			= 0x00008000,
#define SBITS_MISC_BITS				  0x0000f000
};


typedef struct shaderpass_s {
	int numMergedPasses;

#ifndef NOMEDIA
	struct cin_s *cin;
#endif
	
	unsigned int	shaderbits;

	unsigned int	blendmode;

	enum {
		RGB_GEN_WAVE,
		RGB_GEN_ENTITY,
		RGB_GEN_ONE_MINUS_ENTITY,
		RGB_GEN_VERTEX,
		RGB_GEN_EXACT_VERTEX,
		RGB_GEN_ONE_MINUS_VERTEX,
		RGB_GEN_IDENTITY_LIGHTING,
		RGB_GEN_IDENTITY,
		RGB_GEN_CONST,
		RGB_GEN_UNKNOWN,
		RGB_GEN_LIGHTING_DIFFUSE,
		RGB_GEN_TOPCOLOR,
		RGB_GEN_BOTTOMCOLOR
	} rgbgen;
	shaderfunc_t rgbgen_func;

	enum {
		ALPHA_GEN_ENTITY,
		ALPHA_GEN_WAVE,
		ALPHA_GEN_PORTAL,
		ALPHA_GEN_SPECULAR,
		ALPHA_GEN_IDENTITY,
		ALPHA_GEN_VERTEX,
		ALPHA_GEN_CONST
	} alphagen;
	shaderfunc_t alphagen_func;

	enum {
		TC_GEN_BASE,	//basic specified texture coords
		TC_GEN_LIGHTMAP,	//use loaded lightmap coords
		TC_GEN_ENVIRONMENT,
		TC_GEN_DOTPRODUCT,
		TC_GEN_VECTOR,

		//these are really for use only in glsl stuff.
		TC_GEN_NORMAL,
		TC_GEN_SVECTOR,
		TC_GEN_TVECTOR,
	} tcgen;
	int numtcmods;
	tcmod_t		tcmods[SHADER_MAX_TC_MODS];

	int anim_numframes;
	texid_t			anim_frames[SHADER_MAX_ANIMFRAMES];
	float anim_fps;
//	unsigned int texturetype;

	enum {
		T_GEN_SINGLEMAP,	//single texture specified in the shader
		T_GEN_ANIMMAP,		//animating sequence of textures specified in the shader
		T_GEN_LIGHTMAP,		//world light samples
		T_GEN_DELUXMAP,		//world light directions
		T_GEN_SHADOWMAP,	//light's depth values.

		T_GEN_DIFFUSE,		//texture's default diffuse texture
		T_GEN_NORMALMAP,	//texture's default normalmap
		T_GEN_SPECULAR,		//texture's default specular texture
		T_GEN_UPPEROVERLAY,	//texture's default personal colour
		T_GEN_LOWEROVERLAY,	//texture's default team colour
		T_GEN_FULLBRIGHT,	//texture's default fullbright overlay

		T_GEN_CURRENTRENDER,//copy the current screen to a texture, and draw that

		T_GEN_VIDEOMAP,		//use the media playback as an image source, updating each frame for which it is visible
	} texgen;

	enum {
		SHADER_PASS_NOCOLORARRAY = 1<< 3,

		//FIXME: remove these
		SHADER_PASS_VIDEOMAP	= 1 << 4,
		SHADER_PASS_DETAIL		= 1 << 5,
		SHADER_PASS_LIGHTMAP	= 1 << 6,
		SHADER_PASS_DELUXMAP	= 1 << 7,
		SHADER_PASS_ANIMMAP		= 1 << 8
	} flags;
} shaderpass_t;

typedef struct
{
	mesh_t			meshes[5];

	texid_t			farbox_textures[6];
	texid_t			nearbox_textures[6];
} skydome_t;

typedef struct {
	enum shaderprogparmtype_e {
		SP_BAD,

		SP_ENTCOLOURS,
		SP_TOPCOLOURS,
		SP_BOTTOMCOLOURS,
		SP_TIME,
		SP_EYEPOS,
		SP_ENTMATRIX,

		SP_LIGHTRADIUS,
		SP_LIGHTCOLOUR,
		SP_LIGHTPOSITION,

		//things that are set immediatly
		SP_FIRSTIMMEDIATE,	//never set
		SP_CVARI,
		SP_CVARF,
		SP_TEXTURE
	} type;
	unsigned int handle;
} shaderprogparm_t;

typedef struct {
	float factor;
	float unit;
} polyoffset_t;
struct shader_s
{
	int uses;
	int width;
	int height;
	int numpasses;
	texnums_t defaulttextures;
	struct shader_s *next;
	char name[MAX_QPATH];
	//end of shared fields.

	byte_vec4_t fog_color;
	float fog_dist;
	float portaldist;

	int numdeforms;
	deformv_t	deforms[SHADER_DEFORM_MAX];

	polyoffset_t polyoffset;

	enum {
		SHADER_SKY				= 1 << 0,
		SHADER_NOMIPMAPS		= 1 << 1,
		SHADER_NOPICMIP			= 1 << 2,
		SHADER_CULL_FRONT		= 1 << 3,
		SHADER_CULL_BACK		= 1 << 4,
		SHADER_DEFORMV_BULGE	= 1 << 5,
		SHADER_AUTOSPRITE		= 1 << 6,
		SHADER_FLARE			= 1 << 7,
//		SHADER_REMOVED			= 1 << 8,
		SHADER_ENTITY_MERGABLE	= 1 << 9,
		SHADER_VIDEOMAP			= 1 << 10,
		SHADER_DEPTHWRITE		= 1 << 11,
		SHADER_AGEN_PORTAL		= 1 << 12,
		SHADER_BLEND			= 1 << 13,	//blend or alphatest (not 100% opaque).
		SHADER_NODRAW			= 1 << 14,	//parsed only to pee off developers when they forget it on no-pass shaders.

		SHADER_NODLIGHT			= 1 << 15,	//from surfaceflags
		SHADER_HASLIGHTMAP		= 1 << 16
	} flags;

	union {
		int glsl;
	} programhandle;
	int numprogparams;
	shaderprogparm_t progparm[SHADER_PROGPARMS_MAX];

	shaderpass_t passes[SHADER_PASS_MAX];

	shadersort_t sort;

	skydome_t	*skydome;
	shader_gen_t *generator;
	const char	*genargs;

	meshfeatures_t features;
	bucket_t bucket;
};

extern shader_t	*r_shaders;
extern int be_maxpasses;


void R_UnloadShader(shader_t *shader);
shader_t *R_RegisterPic (char *name);
shader_t *R_RegisterShader (char *name, const char *shaderscript);
shader_t *R_RegisterShader_Lightmap (char *name);
shader_t *R_RegisterShader_Vertex (char *name);
shader_t *R_RegisterShader_Flare (char *name);
shader_t *R_RegisterSkin (char *name);
shader_t *R_RegisterCustom (char *name, shader_gen_t *defaultgen, const void *args);
void R_BuildDefaultTexnums(texnums_t *tn, shader_t *shader);

cin_t *R_ShaderGetCinematic(shader_t *s);
cin_t *R_ShaderFindCinematic(char *name);

void Shader_DefaultSkinShell(char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPLM(char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPQ1(char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPQ2(char *shortname, shader_t *s, const void *args);
void Shader_DefaultSkybox(char *shortname, shader_t *s, const void *args);
void Shader_DefaultCinematic(char *shortname, shader_t *s, const void *args);
void Shader_DefaultScript(char *shortname, shader_t *s, const void *args);

void Shader_DoReload(void);
void R_BackendInit (void);
void Shader_Shutdown (void);
qboolean Shader_Init (void);
void Shader_NeedReload(void);

mfog_t *CM_FogForOrigin(vec3_t org);

//not all modes accept meshes - STENCIL(intentional) and DEPTHONLY(not implemented)
typedef enum
{
	BEM_STANDARD,		//regular mode to draw surfaces akin to q3 (aka: legacy mode). lightmaps+delux+ambient
	BEM_DEPTHONLY,		//just a quick depth pass. textures used only for alpha test (shadowmaps).
	BEM_STENCIL,		//used for drawing shadow volumes to the stencil buffer.
	BEM_DEPTHDARK,		//a quick depth pass. textures used only for alpha test. additive textures still shown as normal.
	BEM_LIGHT,			//we have a valid light
	BEM_SMAPLIGHTSPOT,	//we have a spot light using a shadowmap
	BEM_SMAPLIGHT		//we have a light using a shadowmap
} backendmode_t;

#define BEF_FORCEDEPTHWRITE		1
#define BEF_FORCEDEPTHTEST		2
#define BEF_FORCEADDITIVE		4	//blend dest = GL_ONE
#define BEF_FORCETRANSPARENT	8	//texenv replace -> modulate
#define BEF_FORCENODEPTH		16	//disables any and all depth.
#define BEF_PUSHDEPTH			32	//additional polygon offset

//Select the current render mode and modifier flags
void BE_SelectMode(backendmode_t mode, unsigned int flags);

/*Draws an entire mesh list from a VBO. vbo can be null, in which case the chain may be drawn without batching.
  Rules for using a list: Every mesh must be part of the same VBO, shader, lightmap, and must have the same pointers set*/
void BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums);
void BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums);

//Asks the backend to invoke DrawMeshChain for each surface, and to upload lightmaps as required
void BE_DrawWorld (qbyte *vis);

//called at init, force the display to the right defaults etc
void BE_Init(void);

//Generates an optimised VBO, one for each texture on the map
void BE_GenBrushModelVBO(model_t *mod);
//Destroys the given vbo
void BE_ClearVBO(vbo_t *vbo);
//Uploads all modified lightmaps
void BE_UploadAllLightmaps(void);

#ifdef RTLIGHTS
void BE_PushOffsetShadow(qboolean foobar);
//submits the world and ents... used only by gl_shadows.c
void BE_SubmitMeshes (void);
//sets up gl for depth-only FIXME
void BE_SetupForShadowMap(void);
//Called from shadowmapping code into backend
void BE_BaseEntTextures(void);
//Draws lights, called from the backend
void Sh_DrawLights(qbyte *vis);
void Sh_Shutdown(void);
//Draws the depth of ents in the world near the current light
void BE_BaseEntShadowDepth(void);
//Sets the given light+colour to be the current one that everything is to be lit/culled by.
void BE_SelectDLight(dlight_t *dl, vec3_t colour);
//Returns true if the mesh is not lit by the current light
qboolean BE_LightCullModel(vec3_t org, model_t *model);
#endif
#endif
