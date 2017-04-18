/*
Copyright spike. GNU GPL V2. etc.
Much of this file and the parser derives originally from qfusion by vic.

Quake1 rendering works by:
Draw everything in depth order and stall lots from switching textures.
draw transparent water surfaces last.

Quake3 rendering works by:
generate a batch for every model+shader in the world.
sort batches by shader sort key, entity, shader.
draw surfaces.

Doom3 rendering works by:
generate a batch for every model+shader in the world.
sort batches by shader sort key, entity, shader.
depth is drawn (yay alpha masked surfaces)
for each light+batch
	draw each bump/diffuse/specular stage. combine to one pass if that ordering is not maintained. switch diffuse/specular if needed
ambient stages from each batch are added over the top.

FTE rtlight rendering works by:
generate a batch for every model+shader in the world.
sort batches by shader sort key, entity, shader.
draw surfaces. if rtworld_lightmaps is 0 and there's no additive stuff, draw as black, otherwise just scale lightmap passes.
lights are then added over the top based upon the diffusemap, bumpmap and specularmap, and without any pass-specific info (no tcmods).
*/


#ifndef SHADER_H
#define SHADER_H
typedef void (shader_gen_t)(const char *name, shader_t*, const void *args);

#define SHADER_TMU_MAX 16
#define SHADER_PASS_MAX	16
#define SHADER_MAX_TC_MODS	8
#define SHADER_DEFORM_MAX	8
#define SHADER_MAX_ANIMFRAMES	16

typedef enum {
	SHADER_BSP,
	SHADER_BSP_VERTEX,
	SHADER_BSP_FLARE,
	SHADER_MD3,
	SHADER_2D
} shadertype_t;

/*
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
*/

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

	SBITS_MASK_RED						= 0x00000100,
	SBITS_MASK_GREEN					= 0x00000200,
	SBITS_MASK_BLUE						= 0x00000400,
	SBITS_MASK_ALPHA					= 0x00000800,
#define SBITS_MASK_BITS				  0x00000f00

	SBITS_ATEST_NONE					= 0x00000000,
	SBITS_ATEST_GT0						= 0x00001000,
	SBITS_ATEST_LT128					= 0x00002000,
	SBITS_ATEST_GE128					= 0x00003000,
#define SBITS_ATEST_BITS				  0x0000f000
#define SBITS_ATEST_SHIFT				  12

	SBITS_MISC_DEPTHWRITE				= 0x00010000,
	SBITS_MISC_NODEPTHTEST				= 0x00020000,
	SBITS_MISC_DEPTHEQUALONLY			= 0x00040000,
	SBITS_MISC_DEPTHCLOSERONLY			= 0x00080000,
//#define SBITS_MISC_BITS				  0x000f0000

	SBITS_TESSELLATION					= 0x00100000,
	SBITS_AFFINE						= 0x00200000,

	//provided for the backend to hack about with
	SBITS_LINES							= 0x80000000
};


typedef struct shaderpass_s {
	int numMergedPasses;

	struct programshared_s *prog;

#ifdef HAVE_MEDIA_DECODER
	struct cin_s *cin;
#endif
	
	unsigned int	shaderbits;

	enum {
		PBM_MODULATE,
		PBM_OVERBRIGHT,
		PBM_DECAL,
		PBM_ADD,
		PBM_DOTPRODUCT,
		PBM_REPLACE,
		PBM_REPLACELIGHT,
		PBM_MODULATE_PREV_COLOUR
	} blendmode;

	enum {
		RGB_GEN_WAVE,
		RGB_GEN_ENTITY,
		RGB_GEN_ONE_MINUS_ENTITY,
		RGB_GEN_VERTEX_LIGHTING,
		RGB_GEN_VERTEX_EXACT,
		RGB_GEN_ONE_MINUS_VERTEX,
		RGB_GEN_IDENTITY_LIGHTING,
		RGB_GEN_IDENTITY_OVERBRIGHT,
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

		//these are really for use only in glsl stuff or perhaps cubemaps, as they generate 3d coords.
		TC_GEN_NORMAL,
		TC_GEN_SVECTOR,
		TC_GEN_TVECTOR,
		TC_GEN_SKYBOX,
		TC_GEN_WOBBLESKY,
		TC_GEN_REFLECT,

		TC_GEN_UNSPECIFIED
	} tcgen;
	vec3_t tcgenvec[2];	//bloat :(
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
		T_GEN_LIGHTCUBEMAP,	//light's projected cubemap

		T_GEN_DIFFUSE,		//texture's default diffuse texture
		T_GEN_NORMALMAP,	//texture's default normalmap
		T_GEN_SPECULAR,		//texture's default specular texture
		T_GEN_UPPEROVERLAY,	//texture's default personal colour
		T_GEN_LOWEROVERLAY,	//texture's default team colour
		T_GEN_FULLBRIGHT,	//texture's default fullbright overlay
		T_GEN_PALETTED,		//texture's original paletted data (8bit)
		T_GEN_REFLECTCUBE,	//dpreflectcube
		T_GEN_REFLECTMASK,	//dpreflectcube mask

		T_GEN_CURRENTRENDER,//copy the current screen to a texture, and draw that

		T_GEN_SOURCECOLOUR, //used for render-to-texture targets
		T_GEN_SOURCEDEPTH,	//used for render-to-texture targets

		T_GEN_REFLECTION,	//reflection image (mirror-as-fbo)
		T_GEN_REFRACTION,	//refraction image (portal-as-fbo)
		T_GEN_REFRACTIONDEPTH,	//refraction image (portal-as-fbo)
		T_GEN_RIPPLEMAP,	//ripplemap image (water surface distortions-as-fbo)

		T_GEN_SOURCECUBE,	//used for render-to-texture targets

		T_GEN_VIDEOMAP,		//use the media playback as an image source, updating each frame for which it is visible
		T_GEN_CUBEMAP,		//use a cubemap instead, otherwise like T_GEN_SINGLEMAP
		T_GEN_3DMAP			//use a 3d texture instead, otherwise T_GEN_SINGLEMAP.
	} texgen;

	enum {
		ST_DIFFUSEMAP,
		ST_AMBIENT,
		ST_BUMPMAP,
		ST_SPECULARMAP
	} stagetype;

	enum {
		SHADER_PASS_CLAMP		= 1<<0,	//needed for d3d's sampler states, MUST MATCH IMAGE FLAGS
		SHADER_PASS_NOMIPMAP    = 1<<1,	//needed for d3d's sampler states, MUST MATCH IMAGE FLAGS
		SHADER_PASS_NEAREST		= 1<<2,	//needed for d3d's sampler states, MUST MATCH IMAGE FLAGS
		SHADER_PASS_LINEAR		= 1<<3,	//needed for d3d's sampler states, MUST MATCH IMAGE FLAGS
		SHADER_PASS_UIPIC		= 1<<4, //                                 MUST MATCH IMAGE FLAGS
#define SHADER_PASS_IMAGE_FLAGS	(SHADER_PASS_CLAMP|SHADER_PASS_NOMIPMAP|SHADER_PASS_NEAREST|SHADER_PASS_LINEAR|SHADER_PASS_UIPIC)

		SHADER_PASS_DEPTHCMP	= 1<<5,	//needed for d3d's sampler states
		SHADER_PASS_NOCOLORARRAY = 1<<6,

		//FIXME: remove these
		SHADER_PASS_VIDEOMAP	= 1 << 7,
		SHADER_PASS_DETAIL		= 1 << 8,
		SHADER_PASS_LIGHTMAP	= 1 << 9,
		SHADER_PASS_DELUXMAP	= 1 << 10,
		SHADER_PASS_ANIMMAP		= 1 << 11
	} flags;

#if defined(D3D11QUAKE) || defined(VKQUAKE)
	void *becache;	//cache for blendstate objects.
#endif
} shaderpass_t;

typedef struct
{
	texid_t			farbox_textures[6];
	texid_t			nearbox_textures[6];
} skydome_t;

enum{
	#define PERMUTATION_GENERIC	0
	#define PERMUTATION_BUMPMAP			(1u<<PERMUTATION_BIT_BUMPMAP)
	PERMUTATION_BIT_BUMPMAP,			//FIXME: make argument somehow
	#define PERMUTATION_FULLBRIGHT		(1u<<PERMUTATION_BIT_FULLBRIGHT)
	PERMUTATION_BIT_FULLBRIGHT,			//FIXME: make argument somehow
	#define PERMUTATION_UPPERLOWER		(1u<<PERMUTATION_BIT_UPPERLOWER)
	PERMUTATION_BIT_UPPERLOWER,			//FIXME: make argument somehow
	#define PERMUTATION_REFLECTCUBEMASK	(1u<<PERMUTATION_BIT_REFLECTCUBEMASK)
	PERMUTATION_BIT_REFLECTCUBEMASK,	//FIXME: make argument somehow
	#ifdef SKELETALMODELS
		#define PERMUTATION_SKELETAL	(1u<<PERMUTATION_BIT_SKELETAL)
		PERMUTATION_BIT_SKELETAL,
	#else
		#define PERMUTATION_SKELETAL	0u
	#endif
	#define PERMUTATION_FOG				(1u<<PERMUTATION_BIT_FOG)
	PERMUTATION_BIT_FOG,				//FIXME: remove (recompile shaders if its enabled).
	#ifdef NONSKELETALMODELS
		#define PERMUTATION_FRAMEBLEND	(1u<<PERMUTATION_BIT_FRAMEBLEND)
		PERMUTATION_BIT_FRAMEBLEND,
	#else
		#define PERMUTATION_FRAMEBLEND	0u
	#endif
	#if MAXRLIGHTMAPS > 1
		#define PERMUTATION_LIGHTSTYLES	(1u<<PERMUTATION_BIT_LIGHTSTYLES)
		PERMUTATION_BIT_LIGHTSTYLES,	//FIXME: make argument
	#else
		#define PERMUTATION_LIGHTSTYLES	0u
	#endif

	PERMUTATION_BIT_MAX
};
#define PERMUTATIONS				(1u<<PERMUTATION_BIT_MAX)

enum shaderattribs_e
{
	//GLES2 has a limit of 8.
	//GL2 has a limit of 16.
	//vendors may provide more.
	VATTR_VERTEX1=0,	//NOTE: read the comment about VATTR_LEG_VERTEX
	VATTR_VERTEX2=1,
	VATTR_COLOUR=2,
	VATTR_TEXCOORD=3,
	VATTR_LMCOORD=4,
	VATTR_NORMALS=5,
	VATTR_SNORMALS=6,
	VATTR_TNORMALS=7,
	VATTR_BONENUMS=8, /*skeletal only*/
	VATTR_BONEWEIGHTS=9, /*skeletal only*/
#if MAXRLIGHTMAPS > 1
	VATTR_LMCOORD2=10,
	VATTR_LMCOORD3=11,
	VATTR_LMCOORD4=12,
	VATTR_COLOUR2=13,
	VATTR_COLOUR3=14,
	VATTR_COLOUR4=15,
#endif


	VATTR_LEG_VERTEX,	//note: traditionally this is actually index 0.
						//however, implementations are allowed to directly alias, or remap,
						//so we're never quite sure if 0 is enabled or not when using legacy functions.
						//as a result, we use legacy verticies always and never custom attribute 0 if we have any fixed function support.
						//we then depend upon gl_Vertex always being supported by the glsl compiler.
						//this is likely needed anyway to ensure that ftransform works properly and in all cases for stencil shadows.
	VATTR_LEG_COLOUR,
	VATTR_LEG_ELEMENTS,
	VATTR_LEG_TMU0,


	VATTR_LEG_FIRST=VATTR_LEG_VERTEX
};

typedef struct {
	enum shaderprogparmtype_e {
		SP_BAD,	//never set (hopefully)

		/*entity properties*/
		SP_E_VBLEND,
		SP_E_LMSCALE,	//lightmap scales
		SP_E_VLSCALE,	//vertex lighting style scales
		SP_E_ORIGIN,
		SP_E_COLOURS,
		SP_E_COLOURSIDENT,
		SP_E_GLOWMOD,
		SP_E_TOPCOLOURS,
		SP_E_BOTTOMCOLOURS,
		SP_E_TIME,
		SP_E_L_DIR, /*these light values are non-dynamic light as in classic quake*/
		SP_E_L_MUL,
		SP_E_L_AMBIENT,
		SP_E_EYEPOS, /*viewer's eyepos, in model space*/
		SP_V_EYEPOS, /*viewer's eyepos, in world space*/
		SP_W_FOG,

		SP_M_ENTBONES,
		SP_M_VIEW,
		SP_M_MODEL,
		SP_M_MODELVIEW,
		SP_M_PROJECTION,
		SP_M_MODELVIEWPROJECTION,
		SP_M_INVVIEWPROJECTION,
		SP_M_INVMODELVIEWPROJECTION,

		SP_RENDERTEXTURESCALE,	/*multiplier for currentrender->texcoord*/
		SP_SOURCESIZE,			/*size of $sourcecolour*/

		SP_S_COLOUR,

		SP_LIGHTRADIUS, /*these light values are realtime lighting*/
		SP_LIGHTCOLOUR,
		SP_LIGHTCOLOURSCALE,
		SP_LIGHTPOSITION,
		SP_LIGHTSCREEN,
		SP_LIGHTCUBEMATRIX,
		SP_LIGHTSHADOWMAPPROJ,
		SP_LIGHTSHADOWMAPSCALE,

		//things that are set immediatly
		SP_FIRSTIMMEDIATE,	//never set
		SP_CONSTI,
		SP_CONSTF,
		SP_CVARI,
		SP_CVARF,
		SP_CVAR3F,
		SP_TEXTURE
	} type;
	union
	{
		int ival;
		float fval;
		void *pval;
	};
	unsigned int handle;
} shaderprogparm_t;

typedef struct programshared_s
{
	int refs;
	qboolean nofixedcompat;
	qboolean tess;
	unsigned short numsamplers;	//shader system can strip any passes above this
	unsigned int defaulttextures;	//diffuse etc

	unsigned int supportedpermutations;
#ifdef VKQUAKE
	unsigned char *cvardata;
	unsigned int cvardatasize;
	VkRetardedShaderModule vert;		//for slightly faster regeneration
	VkRetardedShaderModule frag;
	VkRetardedPipelineLayout layout;	//all permutations share the same layout. I'm too lazy not to.
	VkRetardedDescriptorSetLayout desclayout;
	struct pipeline_s *pipelines;
#endif
#if defined(GLQUAKE) || defined(D3DQUAKE)
	struct programpermu_s
	{
		union programhandle_u
		{
			qintptr_t loaded;	//generic code must be able to test this to see if its valid. if not 0, then its considered loaded
		#ifdef GLQUAKE
			struct
			{
				int handle;
				qboolean usetesselation;
			} glsl;
		#endif
		#ifdef D3DQUAKE
			struct
			{
				void *vert;
				void *frag;
				#ifdef D3D9QUAKE
					void *ctabf;
					void *ctabv;
				#endif
				#ifdef D3D11QUAKE
					int topology;	//D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
					void *hull;
					void *domain;
					void *geom;
					void *layout;
				#endif
			} hlsl;
		#endif
		} h;
		unsigned int attrmask;
		unsigned int texmask;	//'standard' textures that are in use
		unsigned int numparms;
		shaderprogparm_t *parm;
	} permu[PERMUTATIONS];
#endif
} program_t;

typedef struct {
	float factor;
	float unit;
} polyoffset_t;

enum
{
	LSHADER_STANDARD=0u,	//stencil or shadowless
	LSHADER_CUBE=1u<<0,		//has a cubemap
	LSHADER_SMAP=1u<<1,		//filter based upon 6 directions of a shadowmap
	LSHADER_SPOT=1u<<2,		//filter based upon a single spotlight shadowmap
	LSHADER_MODES=1u<<3
};
enum
{
	//low numbers are used for various rtlight mode combinations
	bemoverride_crepuscular = LSHADER_MODES,	//either black (non-sky) or a special crepuscular_sky shader
	bemoverride_depthonly,		//depth masked. replace if you want alpha test.
	bemoverride_depthdark,		//itself or a pure-black shader. replace for alpha test.
	bemoverride_gbuffer,		//prelighting
	bemoverride_fog,			//post-render volumetric fog
	bemoverride_max
};
//FIXME: split into separate materials and shaders
struct shader_s
{
	char name[MAX_QPATH];
	enum {
		SUF_NONE		= 0,
		SUF_LIGHTMAP	= 1<<0,	//$lightmap passes are valid. otherwise collapsed to an rgbgen
		SUF_2D			= 1<<1,	//any loaded textures will obey 2d picmips rather than 3d picmips
		SUR_FORCEFALLBACK = 1<<2//shader fallback is forced, will not load from disk
	} usageflags;	//
	int uses;	//released when the uses drops to 0
	int width;	//when used as an image, this is the logical 'width' of the image. FIXME.
	int height;
	int numpasses;
	unsigned int numdefaulttextures;	//0 is effectively 1.
	float	defaulttextures_fps;
	texnums_t *defaulttextures;	//must always have at least one entry. multiple will only appear if the diffuse texture was animmapped.
	struct shader_s *next;
	int id;

	rshader_t *bemoverrides[bemoverride_max];
	material_t *remapto;	//render using this material instead. for q3 nonsense.
	float	remaptime;

	byte_vec4_t fog_color;
	float fog_dist;
	float portaldist;

	int numdeforms;
	deformv_t	deforms[SHADER_DEFORM_MAX];

	polyoffset_t polyoffset;

	#define SHADER_CULL_FLIP (SHADER_CULL_FRONT|SHADER_CULL_BACK)
	enum {
		SHADER_SKY				= 1 << 0,
		SHADER_NOMIPMAPS		= 1 << 1,
		SHADER_NOPICMIP			= 1 << 2,
		SHADER_CULL_FRONT		= 1 << 3,
		SHADER_CULL_BACK		= 1 << 4,
		SHADER_NOMARKS			= 1 << 5,
		SHADER_POLYGONOFFSET	= 1 << 6,
		SHADER_FLARE			= 1 << 7,
		SHADER_NEEDSARRAYS		= 1 << 8,	//shader uses deforms or rgbmod tcmods or something that will not work well with sparse vbos
		SHADER_ENTITY_MERGABLE	= 1 << 9,
		SHADER_VIDEOMAP			= 1 << 10,
		SHADER_DEPTHWRITE		= 1 << 11,	//some pass already wrote depth. not used by the renderer.
		SHADER_AGEN_PORTAL		= 1 << 12,
		SHADER_BLEND			= 1 << 13,	//blend or alphatest (not 100% opaque).
		SHADER_NODRAW			= 1 << 14,	//parsed only to pee off developers when they forget it on no-pass shaders.

		SHADER_NODLIGHT			= 1 << 15,	//from surfaceflags
		SHADER_HASLIGHTMAP		= 1 << 16,
		SHADER_HASTOPBOTTOM		= 1 << 17,
//		SHADER_STATICDATA		= 1 << 18,	//set if true: no deforms, no tcgen, rgbgen=identitylighting, alphagen=identity, tmu0=st + tmu1=lm(if available) for every pass, no norms
		SHADER_HASREFLECT		= 1 << 19,	//says that we need to generate a reflection image first
		SHADER_HASREFRACT		= 1 << 20,	//says that we need to generate a refraction image first
		SHADER_HASREFRACTDEPTH	= 1 << 21,	//refraction generation needs to generate a depth texture too.
		SHADER_HASNORMALMAP		= 1 << 22,	//says that we need to load a normalmap texture
		SHADER_HASRIPPLEMAP		= 1 << 23,	//water surface disturbances for water splashes
		SHADER_HASGLOSS			= 1 << 24,	//needs a _spec texture, if possible.
		SHADER_NOSHADOWS		= 1 << 25,	//don't cast shadows
		SHADER_HASFULLBRIGHT	= 1 << 26,	//needs a fullbright texture, if possible.
		SHADER_HASDIFFUSE		= 1 << 27,	//has a T_GEN_DIFFUSE pass
		SHADER_HASPALETTED		= 1 << 28,	//has a T_GEN_PALETTED pass
		SHADER_HASCURRENTRENDER	= 1 << 29,	//has a $currentrender pass
	} flags;

	program_t *prog;

	shaderpass_t passes[SHADER_PASS_MAX];

	shadersort_t sort;

	skydome_t	*skydome;
	shader_gen_t *generator;
	char	*genargs;

	struct shader_clutter_s
	{
		struct shader_clutter_s *next;
		float scalemin;
		float scalemax;
		float anglemin;
		float anglemax;
		float spacing;
		float zofs;
		char modelname[1];
	} *clutter;

	bucket_t bucket;
};

extern unsigned int r_numshaders;
extern unsigned int r_maxshaders;
extern shader_t	**r_shaders;
extern int be_maxpasses;


char *Shader_GetShaderBody(shader_t *s, char *fname, size_t fnamesize);
void R_UnloadShader(shader_t *shader);
int R_GetShaderSizes(shader_t *shader, int *width, int *height, qboolean blocktillloaded);
shader_t *R_RegisterPic (const char *name, const char *subdirs);
shader_t *QDECL R_RegisterShader (const char *name, unsigned int usageflags, const char *shaderscript);
shader_t *R_RegisterShader_Lightmap (const char *name);
shader_t *R_RegisterShader_Vertex (const char *name);
shader_t *R_RegisterShader_Flare (const char *name);
shader_t *QDECL R_RegisterSkin  (const char *shadername, const char *modname);
shader_t *R_RegisterCustom (const char *name, unsigned int usageflags, shader_gen_t *defaultgen, const void *args);
//once loaded, most shaders should have one of the following two calls used upon it
void QDECL R_BuildDefaultTexnums(texnums_t *tn, shader_t *shader);
void QDECL R_BuildLegacyTexnums(shader_t *shader, const char *fallbackname, const char *subpath, unsigned int loadflags, unsigned int imageflags, uploadfmt_t basefmt, size_t width, size_t height, qbyte *mipdata[4], qbyte *palette);
void R_RemapShader(const char *sourcename, const char *destname, float timeoffset);

cin_t *R_ShaderGetCinematic(shader_t *s);
cin_t *R_ShaderFindCinematic(const char *name);
shader_t *R_ShaderFind(const char *name);	//does NOT increase the shader refcount.

void Shader_DefaultSkin(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultSkinShell(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPLM(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPQ1(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPQ2(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultWaterShader(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultSkybox(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultCinematic(const char *shortname, shader_t *s, const void *args);
void Shader_DefaultScript(const char *shortname, shader_t *s, const void *args);

void Shader_ResetRemaps(void);	//called on map changes to reset remapped shaders.
void Shader_DoReload(void);		//called when the shader system dies.
void Shader_Shutdown (void);
qboolean Shader_Init (void);
void Shader_NeedReload(qboolean rescanfs);
void Shader_WriteOutGenerics_f(void);
void Shader_RemapShader_f(void);
void Shader_ShowShader_f(void);

program_t *Shader_FindGeneric(char *name, int qrtype);
void Shader_ReleaseGeneric(program_t *prog);

mfog_t *Mod_FogForOrigin(model_t *wmodel, vec3_t org);

#define BEF_FORCEDEPTHWRITE		1
#define BEF_FORCEDEPTHTEST		2
#define BEF_FORCEADDITIVE		4	//blend dest = GL_ONE
#define BEF_FORCETRANSPARENT	8	//texenv replace -> modulate
#define BEF_FORCENODEPTH		16	//disables any and all depth.
//FIXME: the above should really be legacy-only
#define BEF_PUSHDEPTH			32	//additional polygon offset
#define BEF_NODLIGHT			64  //don't use a dlight pass
#define BEF_NOSHADOWS			128 //don't appear in shadows
#define BEF_FORCECOLOURMOD		256 //q3 shaders default to 'rgbgen identity', and ignore ent colours. this forces ent colours to be considered
#define BEF_LINES				512	//draw line pairs instead of triangles.
#define BEF_FORCETWOSIDED		1024 //more evilness.

typedef struct
{
	int fbo;
	int rb_size[2];
	int rb_depth;
	int rb_stencil;
	int rb_depthstencil;
	texid_t colour;
	unsigned int enables;
} fbostate_t;
#define FBO_RB_DEPTH		2
#define FBO_RB_STENCIL		4
#define FBO_RESET			8	//resize all renderbuffers / free any that are not active. implied if the sizes differ
#define FBO_TEX_DEPTH		32	//internal
#define FBO_TEX_STENCIL		64	//internal

#ifndef __cplusplus	//C++ sucks
typedef struct
{
	char *progpath;	//path to use for glsl/hlsl
	char *blobpath;	//path to use for binary glsl/hlsl blobs.
	char *shadernamefmt;	//optional postfix for shader names for this renderer FIXME: should probably have multiple, for gles to fallback to desktop gl etc.

	qboolean progs_supported;	//can use programs (all but gles1)
	qboolean progs_required;	//no fixed function if this is true (d3d11, gles, gl3core)
	unsigned int minver;		//lowest glsl version usable
	unsigned int maxver;		//highest glsl version usable
	unsigned int max_gpu_bones;	//max number of bones supported by uniforms.

	qboolean texfmt[PTI_MAX];		//which texture formats are supported (renderable not implied)
	unsigned int texture_maxsize;	//max size of a 2d texture
	qboolean texture_non_power_of_two;		//full support for npot
	qboolean texture_non_power_of_two_pic;	//npot only works with clamp-to-edge mipless images.
	qboolean npot_rounddown;				//memory limited systems can say that they want to use less ram.
	qboolean tex_env_combine;
	qboolean nv_tex_env_combine4;
	qboolean env_add;
	qboolean can_mipcap;		//

	void	 (*pDeleteProg)		(program_t *prog);
	qboolean (*pLoadBlob)		(program_t *prog, const char *name, unsigned int permu, vfsfile_t *blobfile);
	qboolean (*pCreateProgram)	(program_t *prog, const char *name, unsigned int permu, int ver, const char **precompilerconstants, const char *vert, const char *tcs, const char *tes, const char *geom, const char *frag, qboolean noerrors, vfsfile_t *blobfile);
	qboolean (*pValidateProgram)(program_t *prog, const char *name, unsigned int permu, qboolean noerrors, vfsfile_t *blobfile);
	void	 (*pProgAutoFields)	(program_t *prog, const char *name, cvar_t **cvars, char **cvarnames, int *cvartypes);
} sh_config_t;
extern sh_config_t sh_config;
#endif
extern const struct sh_defaultsamplers_s
{
	const char *name;
	unsigned int defaulttexbits;
} sh_defaultsamplers[];

#ifdef GLSLONLY
	#define gl_config_nofixedfunc true
#else
	#define gl_config_nofixedfunc gl_config.nofixedfunc
#endif
#ifdef GLESONLY
	#define gl_config_gles true
#else
	#define gl_config_gles gl_config.gles
#endif

#ifdef VKQUAKE
qboolean VK_LoadBlob(program_t *prog, void *blobdata, const char *name);
#endif

#ifdef GLQUAKE
void GLBE_Init(void);
void GLBE_Shutdown(void);
void GLBE_SelectMode(backendmode_t mode);
void GLBE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void GLBE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, unsigned int beflags);
void GLBE_SubmitBatch(batch_t *batch);
batch_t *GLBE_GetTempBatch(void);
void GLBE_GenBrushModelVBO(model_t *mod);
void GLBE_ClearVBO(vbo_t *vbo);
void GLBE_UploadAllLightmaps(void);
void GLBE_DrawWorld (batch_t **worldbatches);
qboolean GLBE_LightCullModel(vec3_t org, model_t *model);
void GLBE_SelectEntity(entity_t *ent);
qboolean GLBE_SelectDLight(dlight_t *dl, vec3_t colour, vec3_t axis[3], unsigned int lmode);
void GLBE_Scissor(srect_t *rect);
void GLBE_SubmitMeshes (batch_t **worldbatches, int start, int stop);
//void GLBE_RenderToTexture(texid_t sourcecol, texid_t sourcedepth, texid_t destcol, texid_t destdepth, qboolean usedepth);
void GLBE_RenderToTextureUpdate2d(qboolean destchanged);
void GLBE_VBO_Begin(vbobctx_t *ctx, size_t maxsize);
void GLBE_VBO_Data(vbobctx_t *ctx, void *data, size_t size, vboarray_t *varray);
void GLBE_VBO_Finish(vbobctx_t *ctx, void *edata, size_t esize, vboarray_t *earray, void **vbomem, void **ebomem);
void GLBE_VBO_Destroy(vboarray_t *vearray, void *mem);

void GLBE_FBO_Sources(texid_t sourcecolour, texid_t sourcedepth);
int GLBE_FBO_Push(fbostate_t *state);
void GLBE_FBO_Pop(int oldfbo);
void GLBE_FBO_Destroy(fbostate_t *state);
int GLBE_FBO_Update(fbostate_t *state, unsigned int enables, texid_t *destcol, int colourbuffers, texid_t destdepth, int width, int height, int layer);
#endif
#ifdef D3D8QUAKE
void D3D8BE_Init(void);
void D3D8BE_Shutdown(void);
void D3D8BE_SelectMode(backendmode_t mode);
void D3D8BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void D3D8BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, unsigned int beflags);
void D3D8BE_SubmitBatch(batch_t *batch);
batch_t *D3D8BE_GetTempBatch(void);
void D3D8BE_GenBrushModelVBO(model_t *mod);
void D3D8BE_ClearVBO(vbo_t *vbo);
void D3D8BE_UploadAllLightmaps(void);
void D3D8BE_DrawWorld (batch_t **worldbatches);
qboolean D3D8BE_LightCullModel(vec3_t org, model_t *model);
void D3D8BE_SelectEntity(entity_t *ent);
qboolean D3D8BE_SelectDLight(dlight_t *dl, vec3_t colour, vec3_t axis[3], unsigned int lmode);
void D3D8BE_VBO_Begin(vbobctx_t *ctx, size_t maxsize);
void D3D8BE_VBO_Data(vbobctx_t *ctx, void *data, size_t size, vboarray_t *varray);
void D3D8BE_VBO_Finish(vbobctx_t *ctx, void *edata, size_t esize, vboarray_t *earray, void **vbomem, void **ebomem);
void D3D8BE_VBO_Destroy(vboarray_t *vearray, void *mem);
void D3D8BE_Scissor(srect_t *rect);

void D3D8Shader_Init(void);
void D3D8BE_Reset(qboolean before);
void D3D8BE_Set2D(void);
#endif
#ifdef D3D9QUAKE
void D3D9BE_Init(void);
void D3D9BE_Shutdown(void);
void D3D9BE_SelectMode(backendmode_t mode);
void D3D9BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void D3D9BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, unsigned int beflags);
void D3D9BE_SubmitBatch(batch_t *batch);
batch_t *D3D9BE_GetTempBatch(void);
void D3D9BE_GenBrushModelVBO(model_t *mod);
void D3D9BE_ClearVBO(vbo_t *vbo);
void D3D9BE_UploadAllLightmaps(void);
void D3D9BE_DrawWorld (batch_t **worldbatches);
qboolean D3D9BE_LightCullModel(vec3_t org, model_t *model);
void D3D9BE_SelectEntity(entity_t *ent);
qboolean D3D9BE_SelectDLight(dlight_t *dl, vec3_t colour, vec3_t axis[3], unsigned int lmode);
void D3D9BE_VBO_Begin(vbobctx_t *ctx, size_t maxsize);
void D3D9BE_VBO_Data(vbobctx_t *ctx, void *data, size_t size, vboarray_t *varray);
void D3D9BE_VBO_Finish(vbobctx_t *ctx, void *edata, size_t esize, vboarray_t *earray, void **vbomem, void **ebomem);
void D3D9BE_VBO_Destroy(vboarray_t *vearray, void *mem);
void D3D9BE_Scissor(srect_t *rect);

void D3D9Shader_Init(void);
void D3D9BE_Reset(qboolean before);
void D3D9BE_Set2D(void);
#endif
#ifdef D3D11QUAKE
void D3D11BE_Init(void);
void D3D11BE_Shutdown(void);
void D3D11BE_SelectMode(backendmode_t mode);
void D3D11BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void D3D11BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, unsigned int beflags);
void D3D11BE_SubmitBatch(batch_t *batch);
batch_t *D3D11BE_GetTempBatch(void);
void D3D11BE_GenBrushModelVBO(model_t *mod);
void D3D11BE_ClearVBO(vbo_t *vbo);
void D3D11BE_UploadAllLightmaps(void);
void D3D11BE_DrawWorld (batch_t **worldbatches);
qboolean D3D11BE_LightCullModel(vec3_t org, model_t *model);
void D3D11BE_SelectEntity(entity_t *ent);
qboolean D3D11BE_SelectDLight(dlight_t *dl, vec3_t colour, vec3_t axis[3], unsigned int lmode);

qboolean D3D11Shader_Init(unsigned int featurelevel);
void D3D11BE_Reset(qboolean before);
void D3D11BE_SetupViewCBuffer(void);
void D3D11_UploadLightmap(lightmapinfo_t *lm);
void D3D11BE_VBO_Begin(vbobctx_t *ctx, size_t maxsize);
void D3D11BE_VBO_Data(vbobctx_t *ctx, void *data, size_t size, vboarray_t *varray);
void D3D11BE_VBO_Finish(vbobctx_t *ctx, void *edata, size_t esize, vboarray_t *earray, void **vbomem, void **ebomem);
void D3D11BE_VBO_Destroy(vboarray_t *vearray, void *mem);
void D3D11BE_Scissor(srect_t *rect);

enum
{	//these are the buffer indexes
	D3D11_BUFF_POS,
	D3D11_BUFF_COL,
	D3D11_BUFF_TC,
	D3D11_BUFF_LMTC,
	D3D11_BUFF_NORM,
	D3D11_BUFF_SKEL,
	D3D11_BUFF_POS2,
	D3D11_BUFF_MAX
};
#endif

//Builds a hardware shader from the software representation
void BE_GenerateProgram(shader_t *shader);

void Sh_RegisterCvars(void);
#ifdef RTLIGHTS
//
void GLBE_PolyOffsetStencilShadow(qboolean foobar);
//sets up gl for depth-only FIXME
int GLBE_SetupForShadowMap(texid_t shadowmaptex, int texwidth, int texheight, float shadowscale);
//Called from shadowmapping code into backend
void GLBE_BaseEntTextures(void);
void D3D9BE_BaseEntTextures(void);
void D3D11BE_BaseEntTextures(void);
//prebuilds shadow volumes
void Sh_PreGenerateLights(void);
//Draws lights, called from the backend
void Sh_DrawLights(qbyte *vis);
void Sh_CheckSettings(void);
void SH_FreeShadowMesh(struct shadowmesh_s *sm);
//frees all memory
void Sh_Shutdown(void);
//resize any textures to match new screen resize
void Sh_Reset(void);
qboolean Sh_StencilShadowsActive(void);
#else
#define Sh_StencilShadowsActive() false
#endif

struct shader_field_names_s
{
	char *name;
	int ptype;
};
extern struct shader_field_names_s shader_field_names[];
extern struct shader_field_names_s shader_unif_names[];
extern struct shader_field_names_s shader_attr_names[];


void CLQ1_DrawLine(shader_t *shader, vec3_t v1, vec3_t v2, float r, float g, float b, float a);
void CLQ1_AddOrientedCube(shader_t *shader, vec3_t mins, vec3_t maxs, float *matrix, float r, float g, float b, float a);
void CL_DrawDebugPlane(float *normal, float dist, float r, float g, float b, qboolean enqueue);
void CLQ1_AddOrientedCylinder(shader_t *shader, float radius, float height, qboolean capsule, float *matrix, float r, float g, float b, float a);
void CLQ1_AddOrientedHalfSphere(shader_t *shader, float radius, float gap, float *matrix, float r, float g, float b, float a);
#endif
